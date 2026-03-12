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
#ifndef VLCICONS_HPP
#define VLCICONS_HPP

#include <QObject>
#include <QString>
#include <QQmlComponent>
#include <QQmlEngine>

#define FONT_FILE(file) \
    static inline const /*constexpr*/ QString fontPath = QStringLiteral(file);
#define FONT_NAME(name) \
    static inline const /*constexpr*/ QString fontName = QStringLiteral(name);
#define FONT_ENTRY(name, path) \
    private: \
        Q_PROPERTY(QString name MEMBER name CONSTANT FINAL) \
    public: \
        static inline const /*constexpr*/ QString name = []() /*constexpr*/ { \
            return encodeNameAdobeLatin1(QStringLiteral(#name)); \
        }();

static inline QString encodeNameAdobeLatin1(QString name)
{
    // https://adobe-type-tools.github.io/adobe-latin-charsets/adobe-latin-1.html
    name.replace('_', QStringLiteral("underscore"));
    name.replace('-', QStringLiteral("hyphen"));
    name.replace('1', QStringLiteral("one"));
    name.replace('2', QStringLiteral("two"));
    name.replace('3', QStringLiteral("three"));
    name.replace('4', QStringLiteral("four"));
    name.replace('5', QStringLiteral("five"));
    name.replace('6', QStringLiteral("six"));
    name.replace('7', QStringLiteral("seven"));
    name.replace('8', QStringLiteral("eight"));
    name.replace('9', QStringLiteral("nine"));
    return name;
}

class VLCIcons : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString fontFamily MEMBER fontName CONSTANT FINAL)
    Q_PROPERTY(QObject* fontLoader MEMBER m_fontLoader CONSTANT FINAL) // Can be downcasted to `QQuickFontLoader`

    FONT_FILE("../pixmaps/VLCIcons.ttf")
    FONT_NAME("VLCIcons")

    FONT_ENTRY(active_indicator, "../pixmaps/icons/active_indicator.svg")
    FONT_ENTRY(add, "../pixmaps/icons/add.svg")
    FONT_ENTRY(album_cover, "../pixmaps/icons/album_cover.svg")
    FONT_ENTRY(aspect_ratio, "../pixmaps/icons/aspect_ratio.svg")
    FONT_ENTRY(atob, "../pixmaps/icons/atob.svg")
    FONT_ENTRY(audiosub, "../pixmaps/icons/audiosub.svg")
    FONT_ENTRY(back, "../pixmaps/icons/back.svg")
    FONT_ENTRY(breadcrumb_sep, "../pixmaps/icons/breadcrumb_sep.svg")
    FONT_ENTRY(breadcrumb_prev, "../pixmaps/icons/breadcrumb_prev.svg")
    FONT_ENTRY(bookmark, "../pixmaps/icons/ic_fluent_bookmark.svg")
    FONT_ENTRY(check, "../pixmaps/icons/check.svg")
    FONT_ENTRY(clear, "../pixmaps/icons/clear.svg")
    FONT_ENTRY(close, "../pixmaps/icons/close.svg")
    FONT_ENTRY(defullscreen, "../pixmaps/icons/ic_fluent_full_screen_minimize.svg")
    FONT_ENTRY(del, "../pixmaps/icons/del.svg")
    FONT_ENTRY(dropzone, "../pixmaps/icons/dropzone.svg")
    FONT_ENTRY(dvd_menu, "../pixmaps/icons/dvd_menu.svg")
    FONT_ENTRY(dvd_next, "../pixmaps/icons/dvd_next.svg")
    FONT_ENTRY(dvd_prev, "../pixmaps/icons/dvd_prev.svg")
    FONT_ENTRY(effect_filter, "../pixmaps/icons/ic_fluent_options.svg")
    FONT_ENTRY(eject, "../pixmaps/icons/ic_fluent_arrow_eject.svg")
    FONT_ENTRY(ellipsis, "../pixmaps/icons/ellipsis.svg")
    FONT_ENTRY(enqueue, "../pixmaps/icons/ic_playlist_add.svg")
    FONT_ENTRY(expand_inverted, "../pixmaps/icons/expand_inverted.svg")
    FONT_ENTRY(expand, "../pixmaps/icons/expand.svg")
    FONT_ENTRY(faster, "../pixmaps/icons/ic_fluent_fast_forward.svg")
    FONT_ENTRY(frame_by_frame, "../pixmaps/icons/frame-by-frame.svg")
    FONT_ENTRY(fullscreen, "../pixmaps/icons/ic_fluent_full_screen_maximize.svg")
    FONT_ENTRY(grid, "../pixmaps/icons/ic_fluent_grid.svg")
    FONT_ENTRY(history, "../pixmaps/icons/history.svg")
    FONT_ENTRY(home, "../pixmaps/icons/home.svg")
    FONT_ENTRY(info, "../pixmaps/icons/info.svg")
    FONT_ENTRY(list, "../pixmaps/icons/ic_fluent_apps_list.svg")
    FONT_ENTRY(more, "../pixmaps/icons/ic_fluent_more_vertical.svg")
    FONT_ENTRY(next, "../pixmaps/icons/ic_fluent_next.svg")
    FONT_ENTRY(ok, "../pixmaps/icons/ok.svg")
    FONT_ENTRY(pause_filled, "../pixmaps/icons/ic_pause_filled.svg")
    FONT_ENTRY(play_filled, "../pixmaps/icons/ic_fluent_play_filled.svg")
    FONT_ENTRY(play, "../pixmaps/icons/ic_fluent_play.svg")
    FONT_ENTRY(ic_fluent_arrow_move, "../pixmaps/icons/ic_fluent_arrow_move.svg")
    FONT_ENTRY(ic_fluent_chevron_down_24, "../pixmaps/icons/ic_fluent_chevron_down_24.svg")
    FONT_ENTRY(ic_fluent_chevron_left_24, "../pixmaps/icons/ic_fluent_chevron_left_24.svg")
    FONT_ENTRY(ic_fluent_chevron_right_24, "../pixmaps/icons/ic_fluent_chevron_right_24.svg")
    FONT_ENTRY(ic_fluent_chevron_up_24, "../pixmaps/icons/ic_fluent_chevron_up_24.svg")
    FONT_ENTRY(play_reverse, "../pixmaps/icons/play_reverse.svg")
    FONT_ENTRY(playlist, "../pixmaps/icons/ic_playlist.svg")
    FONT_ENTRY(playlist_clear, "../pixmaps/icons/ic_playlist_clear.svg")
    FONT_ENTRY(previous, "../pixmaps/icons/ic_fluent_previous.svg")
    FONT_ENTRY(profile_new, "../pixmaps/icons/profile_new.svg")
    FONT_ENTRY(record, "../pixmaps/icons/record.svg")
    FONT_ENTRY(remove, "../pixmaps/icons/remove.svg")
    FONT_ENTRY(renderer, "../pixmaps/icons/renderer.svg")
    FONT_ENTRY(repeat_all, "../pixmaps/icons/ic_fluent_arrow_repeat_all.svg")
    FONT_ENTRY(repeat_one, "../pixmaps/icons/ic_fluent_arrow_repeat_1.svg")
    FONT_ENTRY(search, "../pixmaps/icons/ic_fluent_search.svg")
    FONT_ENTRY(shuffle, "../pixmaps/icons/ic_fluent_arrow_shuffle.svg")
    FONT_ENTRY(skip_back, "../pixmaps/icons/ic_fluent_skip_back_10.svg")
    FONT_ENTRY(skip_for, "../pixmaps/icons/ic_fluent_skip_forward_10.svg")
    FONT_ENTRY(slower, "../pixmaps/icons/ic_fluent_rewind.svg")
    FONT_ENTRY(snapshot, "../pixmaps/icons/ic_fluent_camera.svg")
    FONT_ENTRY(space, "../pixmaps/icons/space.svg")
    FONT_ENTRY(stop, "../pixmaps/icons/ic_fluent_stop.svg")
    FONT_ENTRY(stream, "../pixmaps/icons/stream.svg")
    FONT_ENTRY(time, "../pixmaps/icons/time.svg")
    FONT_ENTRY(topbar_discover, "../pixmaps/icons/ic_fluent_globe.svg")
    FONT_ENTRY(topbar_music, "../pixmaps/icons/ic_fluent_music_note_2.svg")
    FONT_ENTRY(topbar_network, "../pixmaps/icons/ic_fluent_wifi_1.svg")
    FONT_ENTRY(topbar_sort, "../pixmaps/icons/ic_fluent_arrow_sort.svg")
    FONT_ENTRY(topbar_video, "../pixmaps/icons/ic_fluent_filmstrip.svg")
    FONT_ENTRY(chevron_up, "../pixmaps/icons/ic_fluent_chevron_up.svg")
    FONT_ENTRY(chevron_down, "../pixmaps/icons/ic_fluent_chevron_down.svg")
    FONT_ENTRY(tv, "../pixmaps/icons/tv.svg")
    FONT_ENTRY(tvtelx, "../pixmaps/icons/tvtelx.svg")
    FONT_ENTRY(transparency, "../pixmaps/icons/transparency.svg")
    FONT_ENTRY(circle, "../pixmaps/icons/circle.svg")
    FONT_ENTRY(visualization, "../pixmaps/icons/visualization.svg")
    FONT_ENTRY(volume_high, "../pixmaps/icons/volume_high.svg")
    FONT_ENTRY(volume_low, "../pixmaps/icons/volume_low.svg")
    FONT_ENTRY(volume_medium, "../pixmaps/icons/volume_medium.svg")
    FONT_ENTRY(volume_muted, "../pixmaps/icons/volume_muted.svg")
    FONT_ENTRY(volume_zero, "../pixmaps/icons/volume_zero.svg")
    FONT_ENTRY(window_close, "../pixmaps/icons/window_close.svg")
    FONT_ENTRY(window_maximize, "../pixmaps/icons/window_maximize.svg")
    FONT_ENTRY(window_minimize, "../pixmaps/icons/window_minimize.svg")
    FONT_ENTRY(window_restore, "../pixmaps/icons/window_restore.svg")
    FONT_ENTRY(ic_fluent_document_add_24_regular, "../pixmaps/icons/ic_fluent_document_add_24_regular.svg")
    FONT_ENTRY(ic_fluent_document_copy_24_regular, "../pixmaps/icons/ic_fluent_document_copy_24_regular.svg")

public:
    explicit VLCIcons(QObject *parent = nullptr) :
        QObject(parent) { }

    void createFontLoader(QQmlEngine *engine)
    {
        assert(engine);
        QQmlComponent component(engine, nullptr);
        component.setData(QStringLiteral("import QtQuick; FontLoader { source: \"qrc:///%1.ttf\" }").arg(fontName).toLatin1(), {});
        m_fontLoader = component.create(qmlContext(this));
        assert(m_fontLoader);
        m_fontLoader->setParent(this);
    }

private:
    QPointer<QObject> m_fontLoader;
};

Q_GLOBAL_STATIC(VLCIcons, vlcIcons)

#undef FONT_FILE
#undef FONT_NAME
#undef FONT_ENTRY

#endif // VLCICONS_HPP
