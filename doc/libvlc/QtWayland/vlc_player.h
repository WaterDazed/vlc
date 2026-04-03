#pragma once

#include <string>
#include <vlc/libvlc.h>
#include <vlc/libvlc_picture.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_renderer_discoverer.h>
#include <vlc/libvlc_media_player.h>
#include <vlc/vlc.h>
#include <wayland-client.h>
#include <iostream>

class VlcPlayer {
    static inline libvlc_video_output_resize_cb report_size_change = nullptr;
    static inline libvlc_video_output_mouse_move_cb report_mouse_move = nullptr;
    static inline libvlc_video_output_mouse_press_cb report_mouse_press = nullptr;
    static inline libvlc_video_output_mouse_release_cb report_mouse_release = nullptr;
    static inline void* opaque = nullptr;
    

  
    static void set_callbacks(void* data,
            libvlc_video_output_resize_cb report_size_change_,
            libvlc_video_output_mouse_move_cb report_mouse_move_,
            libvlc_video_output_mouse_press_cb report_mouse_press_,
            libvlc_video_output_mouse_release_cb report_mouse_release_,
            void* opaque_) {
        report_size_change = report_size_change_;
        report_mouse_move = report_mouse_move_;
        report_mouse_press = report_mouse_press_;
        report_mouse_release = report_mouse_release_;
        opaque = opaque_;
        
        report_size_change(opaque, width, height);
    }

public:
    VlcPlayer(int argc, const char *const *argv) {
        m_vlc = libvlc_new(argc, argv);
        m_mp = libvlc_media_player_new(m_vlc);
    }

    ~VlcPlayer() {
        libvlc_media_player_release(m_mp);
        libvlc_release(m_vlc);
    }

    void set_render_window(struct wl_display *display, 
                           struct wl_surface *surface) {
        libvlc_media_player_set_wayland_surface(
            m_mp, 
            display, 
            surface, 
            VlcPlayer::set_callbacks
        );
    }

    void open_media(const std::string &path) {
        libvlc_media_t *media = libvlc_media_new_path(path.c_str());
        libvlc_media_player_set_media(m_mp, media);
        libvlc_media_release(media);
    }

    void play() { libvlc_media_player_play(m_mp); }
    
    void set_size(int width, int height) {

  

  
        if (report_size_change != NULL) {
            report_size_change(opaque, width, height);
            VlcPlayer::width = width;
            VlcPlayer::height = height;
        }
    }

    void set_mouse_move(int x, int y) {
        if (report_mouse_move != NULL) {
            report_mouse_move(opaque, x, y);
        }
    }

    void set_mouse_press(libvlc_video_output_mouse_button_t button) {
        if (report_mouse_press != NULL) {
            report_mouse_press(opaque, button);
        }
    }

    void set_mouse_release(libvlc_video_output_mouse_button_t button) {
        if (report_mouse_release != NULL) {
            report_mouse_release(opaque, button);
        }
    }

    static inline int width = 800;
    static inline int height = 600;
private:
    libvlc_instance_t *m_vlc;
    libvlc_media_player_t *m_mp;
};