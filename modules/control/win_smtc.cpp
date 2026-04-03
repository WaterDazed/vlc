/*****************************************************************************
 * win_smtc.cpp : Windows SystemMediaTransportControls integration for VLC
 *****************************************************************************
 * Copyright (c) 2022 Alvin Wong
 *
 * Authors: Alvin Wong
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_es.h>
#include <vlc_interface.h>
#include <vlc_input_item.h>
#include <vlc_messages.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_plugin.h>
#include <vlc_tick.h>
#include <vlc_url.h>

/* C++ standard library headers */
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <span>
#include <string_view>
#include <type_traits>

/* Windows and COM headers */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#if __has_include(<systemmediatransportcontrolsinterop.h>)
# include <systemmediatransportcontrolsinterop.h>
#else
# include <inspectable.h>
// The following are definitions for SystemMediaTransportControlsInterop
// taken from bleeding-edge mingw-w64 (commit 0538a47), and stripped down
// to the minimal required.
extern "C" {
MIDL_INTERFACE("ddb0472d-c911-4a1f-86d9-dc3d71a95f5a")
ISystemMediaTransportControlsInterop : public IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE GetForWindow(
        HWND appWindow,
        REFIID riid,
        void **mediaTransportControl) = 0;
};
# ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(ISystemMediaTransportControlsInterop, 0xddb0472d, 0xc911, 0x4a1f, 0x86,0xd9, 0xdc,0x3d,0x71,0xa9,0x5f,0x5a)
# endif
}
#endif

/* C++/WinRT headers */
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>

using winrt::Windows::Foundation::TimeSpan;
using winrt::Windows::Media::MediaPlaybackStatus;
using winrt::Windows::Media::PlaybackPositionChangeRequestedEventArgs;
using winrt::Windows::Media::SystemMediaTransportControls;
using winrt::Windows::Media::SystemMediaTransportControlsButton;
using winrt::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs;
using winrt::Windows::Media::SystemMediaTransportControlsTimelineProperties;

/**
 * Define how often the SMTC timeline should be updated. MS docs recommends
 * updating every 5 secs.
 */
constexpr vlc_tick_t SMTC_TIMELINE_UPDATE_INTERVAL = VLC_TICK_FROM_MS(5000);

/* Message numbers for private use */
enum {
    WM_MY_QUIT_MSG = WM_USER + 333,
    WM_MY_ARTWORK_UPDATE_MSG,
};

/* Internal state for an instance of the module */
struct intf_sys_t
{
    vlc_thread_t thread{};
    HANDLE ready_event{};
    bool thread_errored{};
    HWND hwnd{};
    vlc_tick_t last_timeline_update_time{};
    SystemMediaTransportControls smtc{ nullptr };
    SystemMediaTransportControlsTimelineProperties timeline_props{ nullptr };
    vlc_playlist_t *playlist{};
    vlc_player_listener_id *player_listener{};
    vlc_playlist_listener_id *playlist_listener{};
};

/* Helper class for handling C strings with RAII */
class owned_cstr
{
    struct deleter {
      void operator()(char *psz_str) { free(psz_str); }
    };

    std::unique_ptr<char, deleter> m_psz_str;
    std::string_view m_string_view;

public:
    owned_cstr() noexcept {}
    owned_cstr(const owned_cstr &) noexcept = delete;
    owned_cstr(owned_cstr &&) noexcept = default;

    explicit owned_cstr(char *psz_str) noexcept
        : m_psz_str{psz_str}
    {
        if (psz_str)
            m_string_view = { psz_str };
    }

    owned_cstr &operator=(const owned_cstr &) noexcept = delete;
    owned_cstr &operator=(owned_cstr &&) noexcept = default;

    operator bool() const noexcept {
        return m_psz_str.get();
    }

    operator const std::string_view &() const noexcept {
        return m_string_view;
    }

    const char *get() const noexcept {
        return m_psz_str.get();
    }

    std::string_view* operator->() noexcept {
        return &m_string_view;
    }

    const char& operator[](size_t pos) noexcept {
        return m_string_view[pos];
    }

    char *release() noexcept {
        m_string_view = {};
        return m_psz_str.release();
    }

    operator winrt::hstring() const {
        if (m_string_view.empty())
            return {};
        // Note: winrt::to_hstring takes UTF-8 strings.
        return winrt::to_hstring(m_string_view);
    }
};

static inline winrt::hstring hstr(const owned_cstr &cstr)
{
    return static_cast<winrt::hstring>(cstr);
}

/**
 * Helper function to log a caught exception. Must be called only from inside
 * a `catch` block, or otherwise will call `std::terminate()`.
 */
static void log_exception(intf_thread_t *intf, const char *func) noexcept try
{
    throw;
} catch (const winrt::hresult_error &ex) {
    msg_Err(intf, "unhandled exception in %s "
            "(hresult %x)", func, ex.code().value);
} catch (const std::exception &ex) {
    msg_Err(intf, "unhandled exception in %s "
            "(%s)", func, ex.what());
} catch (...) {
    msg_Err(intf, "unhandled exception in %s"
            "(unknown)", func);
}

static bool is_playable_media(input_item_t *media)
{
    switch (media->i_type) {
    case ITEM_TYPE_DISC:
    case ITEM_TYPE_FILE:
    case ITEM_TYPE_STREAM:
        return true;
    case ITEM_TYPE_UNKNOWN:
    case ITEM_TYPE_DIRECTORY:
    case ITEM_TYPE_CARD:
    case ITEM_TYPE_PLAYLIST:
    case ITEM_TYPE_NODE:
    default:
        return false;
    }
}

static void update_media_metadata(intf_thread_t *intf, input_item_t *media)
{
    intf_sys_t *sys = intf->p_sys;

    if (!is_playable_media(media))
        return;

    owned_cstr psz_title{ input_item_GetTitleFbName(media) };
    owned_cstr psz_genre{ input_item_GetGenre(media) };
    auto updater = sys->smtc.DisplayUpdater();
    updater.ClearAll();

    // SMTC supports different properties for different media types. Available
    // types are music, video and image. We don't need to handle images. To
    // distinguish video from music, just check if the media contains a video
    // stream.
    const auto esFormats = std::span(media->es, media->i_es);
    bool hasVideo = std::any_of(esFormats.begin(), esFormats.end(),
                                [](auto *es) { return es->i_cat == VIDEO_ES; });
    msg_Dbg(intf, "updating media metadata: hasVideo=%d, title=%s", hasVideo,
            psz_title ? psz_title->begin() : "");
    if (hasVideo) {
        updater.Type(winrt::Windows::Media::MediaPlaybackType::Video);
        auto v = updater.VideoProperties();
        v.Title(hstr(psz_title));
        // TODO: Set v.Subtitle?
        if (!psz_genre->empty())
            v.Genres().Append(hstr(psz_genre));
    } else {
        updater.Type(winrt::Windows::Media::MediaPlaybackType::Music);
        auto m = updater.MusicProperties();
        m.Title(hstr(psz_title));
        m.Artist(hstr(owned_cstr{ input_item_GetArtist(media) }));
        m.AlbumTitle(hstr(owned_cstr{ input_item_GetAlbum(media) }));
        m.AlbumArtist(hstr(owned_cstr{ input_item_GetAlbumArtist(media) }));
        if (!psz_genre->empty())
            m.Genres().Append(hstr(psz_genre));
        // VLC supplies track number and count as strings, but SMTC wants
        // them as integers, so convert them if possible.
        uint32_t trackNumber;
        owned_cstr psz_track_number{ input_item_GetTrackNumber(media) };
        auto res = std::from_chars(psz_track_number->begin(),
                                   psz_track_number->end(), trackNumber);
        if (res.ec == std::errc())
            m.TrackNumber(trackNumber);
        uint32_t trackTotal;
        owned_cstr psz_track_total{ input_item_GetTrackTotal(media) };
        res = std::from_chars(psz_track_total->begin(), psz_track_total->end(),
                              trackTotal);
        if (res.ec == std::errc())
            m.AlbumTrackCount(trackTotal);
    }

    // The metadata is not updated until Update() has been called.
    updater.Update();

    // Loading the artwork is potentially time-consuming, so do it on the
    // plugin thread to not block this thread.
    PostMessageW(sys->hwnd, WM_MY_ARTWORK_UPDATE_MSG, (WPARAM)intf,
                 (LPARAM)input_item_GetArtworkURL(media));
}

static void on_current_media_changed(vlc_player_t *player,
                                     input_item_t *new_media,
                                     void *data) noexcept try
{
    VLC_UNUSED(player);
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    if (!new_media || !is_playable_media(new_media)) {
        msg_Dbg(intf, "no media selected, disabling SMTC");
        sys->smtc.IsEnabled(false);
        return;
    }
    sys->smtc.IsEnabled(true);
    update_media_metadata(intf, new_media);
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static void on_state_changed(vlc_player_t *player, vlc_player_state new_state,
                             void *data) noexcept try
{
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    MediaPlaybackStatus status;
    switch (new_state) {
    case VLC_PLAYER_STATE_STARTED:
    case VLC_PLAYER_STATE_PLAYING:
        status = MediaPlaybackStatus::Playing;
        break;
    case VLC_PLAYER_STATE_PAUSED:
        status = MediaPlaybackStatus::Paused;
        break;
    case VLC_PLAYER_STATE_STOPPING:
        status = MediaPlaybackStatus::Changing;
        break;
    case VLC_PLAYER_STATE_STOPPED:
    default:
        status = MediaPlaybackStatus::Stopped;
        break;
    }
    sys->smtc.PlaybackStatus(status);

    switch (new_state) {
    case VLC_PLAYER_STATE_STOPPED:
    case VLC_PLAYER_STATE_PAUSED: {
        // When playback is paused or stopped, always update the timeline
        // because on_position_changed is not going to be fired again.
        sys->last_timeline_update_time = vlc_tick_now();
        auto time = vlc_player_GetTime(player);
        sys->timeline_props.Position(std::chrono::microseconds(US_FROM_VLC_TICK(time)));
        sys->smtc.UpdateTimelineProperties(sys->timeline_props);
    } break;
    default:
        break;
    }
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static void on_position_changed(vlc_player_t *player, vlc_tick_t new_time,
                                double new_pos, void *data) noexcept try
{
    VLC_UNUSED(player);
    VLC_UNUSED(new_pos);
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    auto now = vlc_tick_now();
    // Update the timeline at a limited rate. Always update if the player
    // is paused.
    if (!vlc_player_IsPaused(player) && sys->last_timeline_update_time &&
        now - sys->last_timeline_update_time < SMTC_TIMELINE_UPDATE_INTERVAL)
        return;
    sys->last_timeline_update_time = now;
    sys->timeline_props.Position(std::chrono::microseconds(US_FROM_VLC_TICK(new_time)));
    sys->smtc.UpdateTimelineProperties(sys->timeline_props);
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static void on_length_changed(vlc_player_t *player, vlc_tick_t new_length,
                              void *data) noexcept try
{
    VLC_UNUSED(player);
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    sys->timeline_props.StartTime(TimeSpan(0));
    sys->timeline_props.MinSeekTime(TimeSpan(0));
    TimeSpan length{0};
    if (new_length > 0)
        length = std::chrono::microseconds(US_FROM_VLC_TICK(new_length));
    sys->timeline_props.MaxSeekTime(length);
    sys->timeline_props.EndTime(length);
    sys->smtc.UpdateTimelineProperties(sys->timeline_props);
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static void on_media_meta_changed(vlc_player_t *player, input_item_t *media,
                                  void *data) noexcept try
{
    VLC_UNUSED(player);
    intf_thread_t *intf = (intf_thread_t *)data;
    update_media_metadata(intf, media);
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static const vlc_player_cbs player_cbs = {
    .on_current_media_changed = on_current_media_changed,
    .on_state_changed = on_state_changed,
    .on_position_changed = on_position_changed,
    .on_length_changed = on_length_changed,
    .on_media_meta_changed = on_media_meta_changed,
};

static void on_playback_repeat_changed(vlc_playlist_t *playlist,
                                       vlc_playlist_playback_repeat repeat,
                                       void *data) noexcept try
{
    using RepeatMode = winrt::Windows::Media::MediaPlaybackAutoRepeatMode;
    VLC_UNUSED(playlist);
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    RepeatMode repeatMode;
    switch (repeat) {
    case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
        repeatMode = RepeatMode::Track;
        break;
    case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
        repeatMode = RepeatMode::List;
        break;
    case VLC_PLAYLIST_PLAYBACK_REPEAT_NONE:
    default:
        repeatMode = RepeatMode::None;
        break;
    }
    sys->smtc.AutoRepeatMode(repeatMode);
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static void on_playback_order_changed(vlc_playlist_t *playlist,
                                      vlc_playlist_playback_order order,
                                      void *data) noexcept try
{
    VLC_UNUSED(playlist);
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    bool isRandom = order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    sys->smtc.ShuffleEnabled(isRandom);
} catch (...) {
    log_exception((intf_thread_t *)data, __func__);
}

static const vlc_playlist_callbacks playlist_cbs = {
    .on_playback_repeat_changed = on_playback_repeat_changed,
    .on_playback_order_changed = on_playback_order_changed,
};

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam) noexcept {
    switch (uMsg) {
    case WM_MY_QUIT_MSG:
        PostQuitMessage(0);
        return 0;
    case WM_MY_ARTWORK_UPDATE_MSG: try {
        intf_thread_t *intf = (intf_thread_t *)wParam;
        if (!intf)
            return 0;
        intf_sys_t *sys = intf->p_sys;
        owned_cstr psz_artwork_path{ [&]() -> char * {
            owned_cstr psz_artwork_url{ (char *)lParam };
            if (psz_artwork_url->empty())
                return nullptr;
            return vlc_uri2path(psz_artwork_url.get());
        }() };
        // Here it is possible for another thread to be updating other
        // properties at the same time, but there should be no real risk of
        // problematic race conditions. Worst case we just call Update() twice.
        auto updater = sys->smtc.DisplayUpdater();
        if (!psz_artwork_path->empty()) {
            using winrt::Windows::Storage::StorageFile;
            using StreamRef = winrt::Windows::Storage::Streams::
                RandomAccessStreamReference;
            msg_Dbg(intf, "updating artwork: %s", psz_artwork_path->begin());
            // This blocks synchronously but should be fine because this is
            // running on the plugin thread. To run this asynchronously, we
            // will need to handle cancellation properly and also risk getting
            // bitten by coroutine bugs in the compiler, so not really worth
            // the effort.
            auto artworkFile =
                StorageFile::GetFileFromPathAsync(hstr(psz_artwork_path)).get();
            // We now check if there are any newer artwork update requests
            // from the thread, so that we can skip updating with the obsolete
            // artwork.
            MSG peekMsg{};
            if (PeekMessageW(&peekMsg, hwnd, WM_MY_ARTWORK_UPDATE_MSG,
                             WM_MY_ARTWORK_UPDATE_MSG, PM_NOREMOVE))
                return 0;
            updater.Thumbnail(StreamRef::CreateFromFile(artworkFile));
        } else {
            msg_Dbg(intf, "updating artwork: (null)");
            updater.Thumbnail(nullptr);
        }
        updater.Update();
    } catch (...) {
        log_exception((intf_thread_t *)wParam, "WM_MY_ARTWORK_UPDATE_MSG");
    } return 0;
    case WM_CLOSE:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

extern "C" IMAGE_DOS_HEADER __ImageBase;

static HWND CreateMessageHwnd() noexcept
{
    HINSTANCE hInstance = (HINSTANCE)&__ImageBase;
    HWND hwnd = CreateWindowExW(
        0,                                          // Optional window styles.
        L"STATIC",                                  // Window class
        L"VLC SystemMediaTransportControls hwnd",   // Window text
        WS_POPUP,                                   // Window style

        // Size and position
        0, 0, 0, 0,

        nullptr,    // Parent window
        nullptr,    // Menu
        hInstance,  // Instance handle
        nullptr     // Additional application data
    );
    if (!hwnd)
        return nullptr;
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);
    return hwnd;
}

static SystemMediaTransportControls GetSmtcForHwnd(HWND hwnd)
{
    auto smtcInterop =
        winrt::get_activation_factory<SystemMediaTransportControls,
                                      ISystemMediaTransportControlsInterop>();
    SystemMediaTransportControls smtc{ nullptr };
    HRESULT hr = smtcInterop->GetForWindow(
        hwnd, winrt::guid_of<SystemMediaTransportControls>(),
        winrt::put_abi(smtc));
    if (FAILED(hr)) {
        winrt::throw_hresult(hr);
    }
    return smtc;
}

/**
 * Plugin thread that runs the message loop.
 */
static void *PluginThread(void *data) noexcept
{
    intf_thread_t *intf = (intf_thread_t *)data;
    intf_sys_t *sys = intf->p_sys;
    vlc_thread_set_name("vlc-smtc-win");

    struct init_error_t {};

    std::unique_ptr<std::remove_pointer_t<HWND>, void (*)(HWND)> hwnd
        { nullptr, [](HWND hwnd) { DestroyWindow(hwnd); } };
    std::unique_ptr<std::remove_pointer_t<HANDLE>, void (*)(HANDLE)>
        timeline_timer{ nullptr, [](HANDLE h) { CloseHandle(h); } };
    SystemMediaTransportControls smtc{ nullptr };

    try {
        winrt::init_apartment();

        hwnd.reset(CreateMessageHwnd());
        if (!hwnd) {
            DWORD lastError = GetLastError();
            msg_Err(intf, "couldn't create window for Windows SMTC plugin "
                    "(error %lu)", lastError);
            throw init_error_t{};
        }

        try {
            smtc = GetSmtcForHwnd(hwnd.get());
            sys->smtc = smtc;
            sys->timeline_props = SystemMediaTransportControlsTimelineProperties{};
        } catch (const winrt::hresult_error &ex) {
            msg_Err(intf, "couldn't initialize SystemMediaTransportControls "
                    "(hresult %x)", ex.code().value);
            throw init_error_t{};
        }

        sys->playlist = vlc_intf_GetMainPlaylist(intf);
        vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
        vlc_player_Lock(player);
        sys->player_listener = vlc_player_AddListener(player, &player_cbs, intf);
        sys->playlist_listener =
            vlc_playlist_AddListener(sys->playlist, &playlist_cbs, intf, true);
        vlc_player_Unlock(player);
        if (!sys->player_listener || !sys->playlist_listener) {
            msg_Err(intf, "couldn't start Windows SMTC plugin "
                    "(failed to add listeners)");
            throw init_error_t{};
        }

        // Note: Changing these dynamically seems to be unreliable.
        smtc.IsPlayEnabled(true);
        smtc.IsPauseEnabled(true);
        smtc.IsStopEnabled(true);
        smtc.IsFastForwardEnabled(true);
        smtc.IsRewindEnabled(true);
        smtc.IsPreviousEnabled(true);
        smtc.IsNextEnabled(true);

        smtc.ButtonPressed(
            [intf](const SystemMediaTransportControls &sender,
                   const SystemMediaTransportControlsButtonPressedEventArgs &args) {
                using Button = SystemMediaTransportControlsButton;
                VLC_UNUSED(sender);
                intf_sys_t *sys = intf->p_sys;
                vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
                vlc_player_Lock(player);
                switch (args.Button()) {
                case Button::Play:
                    if (vlc_player_IsStarted(player)) 
                        vlc_player_Resume(player);
                    else
                        vlc_player_Start(player);
                    break;
                case Button::Pause:
                    vlc_player_Pause(player);
                    break;
                case Button::Stop:
                    vlc_player_Stop(player);
                    break;
                case Button::Next:
                    vlc_playlist_Next(sys->playlist);
                    break;
                case Button::Previous:
                    vlc_playlist_Prev(sys->playlist);
                    break;
                case Button::FastForward:
                case Button::Rewind: {
                    int sign = args.Button() == Button::FastForward ? 1 : -1;
                    int jmpsz = var_InheritInteger(vlc_object_instance(intf),
                                                   "medium-jump-size");
                    if (jmpsz >= 0)
                        vlc_player_JumpTime(player, vlc_tick_from_sec(jmpsz * sign));
                } break;
                default:
                    break;
                }
                vlc_player_Unlock(player);
            });
        smtc.PlaybackPositionChangeRequested(
            [sys](const SystemMediaTransportControls &sender,
                  const PlaybackPositionChangeRequestedEventArgs &args) {
                VLC_UNUSED(sender);
                auto seekPosUsec =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        args.RequestedPlaybackPosition())
                        .count();
                vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
                vlc_player_Lock(player);
                vlc_player_SetTimeFast(player, VLC_TICK_FROM_US(seekPosUsec));
                vlc_player_Unlock(player);
            });
        smtc.AutoRepeatModeChangeRequested(
            [sys](const SystemMediaTransportControls &sender,
                  const winrt::Windows::Media::AutoRepeatModeChangeRequestedEventArgs &args) {
                using RepeatMode =
                    winrt::Windows::Media::MediaPlaybackAutoRepeatMode;
                VLC_UNUSED(sender);
                vlc_playlist_playback_repeat repeat;
                switch (args.RequestedAutoRepeatMode()) {
                case RepeatMode::Track:
                    repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
                    break;
                case RepeatMode::List:
                    repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
                    break;
                case RepeatMode::None:
                default:
                    repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
                    break;
                }
                vlc_playlist_Lock(sys->playlist);
                vlc_playlist_SetPlaybackRepeat(sys->playlist, repeat);
                vlc_playlist_Unlock(sys->playlist);
            });
        smtc.ShuffleEnabledChangeRequested(
            [sys](const SystemMediaTransportControls &sender,
                  const winrt::Windows::Media::ShuffleEnabledChangeRequestedEventArgs &args) {
                VLC_UNUSED(sender);
                auto order = args.RequestedShuffleEnabled()
                                 ? VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM
                                 : VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
                vlc_playlist_Lock(sys->playlist);
                vlc_playlist_SetPlaybackOrder(sys->playlist, order);
                vlc_playlist_Unlock(sys->playlist);
            });

        // Do not enable before we have an active playable media.
        smtc.IsEnabled(false);
    } catch (...) {
        try {
            throw;
        } catch (const winrt::hresult_error &ex) {
            msg_Err(intf, "couldn't initialize Windows SMTC plugin "
                    "(hresult %x)", ex.code().value);
        } catch (const std::exception &ex) {
            msg_Err(intf, "couldn't initialize Windows SMTC plugin "
                    "(unknown exception %s)", ex.what());
        } catch (init_error_t) {
            // ignored
        } catch (...) {
            msg_Err(intf, "couldn't initialize Windows SMTC plugin "
                    "(unknown exception)");
        }
        if (sys->player_listener || sys->playlist_listener) {
            vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
            vlc_player_Lock(player);
            if (sys->player_listener)
                vlc_player_RemoveListener(
                    player, std::exchange(sys->player_listener, nullptr));
            if (sys->playlist_listener)
                vlc_playlist_RemoveListener(
                    sys->playlist,
                    std::exchange(sys->playlist_listener, nullptr));
            vlc_player_Unlock(player);
        }
        sys->thread_errored = true;
        SetEvent(sys->ready_event);
        return nullptr;
    }

    // Initialization completed.
    sys->hwnd = hwnd.get();
    SetEvent(sys->ready_event);

    {
        MSG msg{};
        while (int ret = GetMessageW(&msg, nullptr, 0, 0)) {
            if (ret < 0)
                break;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    sys->hwnd = nullptr;
    return nullptr;
}

/**
 * Starts our plugin.
 */
static int Open(vlc_object_t *obj) noexcept
{
    intf_thread_t *intf = (intf_thread_t *)obj;

    std::unique_ptr<intf_sys_t> sys{ new (std::nothrow) intf_sys_t{} };
    if (unlikely(!sys))
        return VLC_ENOMEM;

    /* Run the plugin thread */
    intf->p_sys = sys.get();
    sys->ready_event = CreateEventW(nullptr, false, false, nullptr);
    if (vlc_clone(&sys->thread, PluginThread, intf)) {
        msg_Err(intf, "couldn't create thread for Windows SMTC plugin");
        return VLC_ENOMEM;
    }

    WaitForSingleObject(sys->ready_event, INFINITE);
    CloseHandle(sys->ready_event);
    if (sys->thread_errored) {
        vlc_join(sys->thread, nullptr);
        return VLC_EGENERIC;
    }

    intf->p_sys = sys.release();
    return VLC_SUCCESS;
}

/**
 * Stops the plugin.
 */
static void Close(vlc_object_t *obj) noexcept
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    std::unique_ptr<intf_sys_t> sys{ std::exchange(intf->p_sys, nullptr) };

    if (sys->hwnd)
        PostMessageW(sys->hwnd, WM_MY_QUIT_MSG, 0, 0);
    vlc_join(sys->thread, nullptr);

    vlc_player_t *player = vlc_playlist_GetPlayer(sys->playlist);
    vlc_player_Lock(player);
    vlc_player_RemoveListener(player, sys->player_listener);
    vlc_playlist_RemoveListener(sys->playlist, sys->playlist_listener);
    vlc_player_Unlock(player);
}

/* Module descriptor */
vlc_module_begin()
    set_shortname("SMTC")
    set_description(N_("System Media Transport Controls (SMTC) integration"))
    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end ()
