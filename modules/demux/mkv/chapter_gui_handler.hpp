// Copyright (C) 2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_gui_handler.hpp: user interaction and display handling for chapter codecs
// Author: Khalid Masum <khalid.masum.92@gmail.com>
// Author: Steve Lhomme <steve.lhomme@free.fr>

#ifndef VLC_MKV_CHAPTER_GUI_HANDLER
#define VLC_MKV_CHAPTER_GUI_HANDLER

#include "mkv.hpp"
#include "matroska_segment.hpp"

#include <vlc_subpicture.h>

namespace mkv {
class mouse_operable {
public:
    virtual unsigned get_x_start() const = 0;
    virtual unsigned get_y_start() const = 0;
    virtual unsigned get_x_end() const = 0;
    virtual unsigned get_y_end() const = 0;

    virtual void do_on_mouse_pressed() = 0;
    virtual void do_on_mouse_over() = 0;

    virtual void update() = 0 ;

    virtual ~mouse_operable(){}
};


class mouse_event_handler {
    std::vector<mouse_operable*> operables;

    unsigned video_height, video_width;
    bool check_inside(const mouse_operable *op, const unsigned x, const unsigned y) const;
    public:

    mouse_event_handler(const unsigned h, const unsigned w)
    {
        video_width = w;
        video_height = h;
    }
    void addMouseOperable(mouse_operable *op);
    void clearMouseOperables();

    bool executeOnMousePressed(const unsigned x, const unsigned y) const;
    void executeOnMouseOver(const unsigned x, const unsigned y) const;

    void update() const;
};

class choice_button;

class choice_palette
{
    bool requires_update = true;
    unsigned video_width, video_height;
    std::vector<choice_button*> button_list;
    virtual_segment_c *sys;

    es_out_id_t *es_out_id;
    es_out_t *es_out;
    size_t channel_id;

    bool is_overlay_shown = false;



    static void UpdateChoice(subpicture_t *p_subpic,
                             const struct vlc_spu_updater_configuration *conf
                             )
    {
        choice_palette *me = static_cast<choice_palette*>(p_subpic->updater.sys);
        me->update();
    }


public:
    video_palette_t button_bg_palette;
    video_format_t bg_fmt;
    chapter_codec_vm::choices *choice_map;

    int width;
    int height;
    int selected;

    int btn_count = 0;

    subpicture_t * subpic;
    subpicture_updater_t updater;


    mouse_event_handler evh;

    choice_palette(unsigned video_height,
                   unsigned video_width,
                   es_out_t *es_out, es_out_id_t *es_out_id
                   );
    void setChoices(chapter_codec_vm::choices *chapter_choices);
    void display_overlay();
    void hide_overlay();

    void create_button (const chapter_codec_vm::choice_uid &uid, const chapter_codec_vm::chapter_choice &choice);
    void clear_buttons();

    void update();
    void try_mouse_click(const unsigned x, const unsigned y);
    void try_mouse_over(const unsigned x, const unsigned y);

    void MarkGroupChoiceSelected(const chapter_codec_vm::choice_uid &uid, const chapter_codec_vm::choice_group &group);

    bool IsChoiceSelected(const chapter_codec_vm::choice_uid &uid, const chapter_codec_vm::choice_group &group);

    ~choice_palette();
};

class choice_button: public mouse_operable
{
    subpicture_region_t *button_bg;
    subpicture_region_t *button_spu;
    choice_palette *palette;
    const std::string txt;
    chapter_codec_vm::chapter_choice choice;
    const chapter_codec_vm::choice_uid &uid;

    unsigned i;
    unsigned start_x, start_y, end_x, end_y;

    void update_dims(const unsigned button_width, const unsigned button_height, unsigned i);
    public:
    unsigned get_x_start() const override;
    unsigned get_y_start() const override;
    unsigned get_x_end() const override;
    unsigned get_y_end() const override;

    choice_button(choice_palette *palette,
                  const chapter_codec_vm::choice_uid &uid,
                  const chapter_codec_vm::chapter_choice &choice,
                  unsigned i)
    :txt(choice.per_language_text.cbegin()->second),
     choice(choice),
     uid(uid)
    {
        this->palette = palette;
        this->i = i;
        update_dims(palette->width, palette->height, i);
    }

    ~choice_button(){}

    void update() override;
    void do_on_mouse_pressed() override;
    void do_on_mouse_over() override;
};

}
#endif
