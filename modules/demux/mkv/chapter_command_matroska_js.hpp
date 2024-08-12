// Copyright (C) 2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_matroska_js.hpp : MatroskaJS implementation for Matroska Chapter Codecs
// Author: Khalid Masum <khalid.masum.92@gmail.com>

#ifndef VLC_MKV_CHAPTER_COMMAND_MATROSKA_JS_HPP_
#define VLC_MKV_CHAPTER_COMMAND_MATROSKA_JS_HPP_

#include <optional>
#include "chapter_command_script_common.hpp"

typedef struct duk_hthread duk_context;
typedef int duk_small_int_t;
typedef unsigned int duk_small_uint_t;
typedef duk_small_int_t duk_ret_t;

namespace mkv {

class matroska_js_interpreter_c : public matroska_script_interpreter_common_c
{
private:

    using choice_uid=std::string;
    using choice_group=chapter_codec_vm::choice_group;
    chapter_codec_vm::choices choice_map;


    bool gotoChapter(chapter_uid i_chapter_uid);

    //JavaScript access functions
    static duk_ret_t js_execute_GotoAndPlay(duk_context *ctx);
    static duk_ret_t js_execute_LogMsg(duk_context *ctx);
    static duk_ret_t js_execute_AddChoice(duk_context *ctx);
    static duk_ret_t js_execute_SetChoiceDefault(duk_context *ctx);
    static duk_ret_t js_execute_SetChoiceText(duk_context *ctx);
    static duk_ret_t js_execute_CommitChoices(duk_context *ctx);
    static duk_ret_t js_execute_GetChoice(duk_context *ctx);

    //Command executors
    bool execute_GotoAndPlay(const std::string &arg);
    bool execute_LogMsg(const std::string &arg);
    bool execute_AddChoice(const std::string &choice_uid, const std::optional <std::string> &choice_group);
    void execute_SetChoiceDefault(const choice_uid &uid, const choice_group &group);
    bool execute_SetChoiceText(const std::string &uid, const std::string &text, const std::string &lang);
    void execute_CommitChoices();
    const std::optional<choice_uid> execute_GetChoice(const choice_group &group);

    duk_context *ctx;

    static void execution_timeout_checker(void *data);
    static void fatal_error_handler(void *udata, const char* msg);
    duk_context* ms_setup();
    void on_timeout();
public:
    std::atomic_bool timed_out;

    matroska_js_interpreter_c( struct vlc_logger *log, chapter_codec_vm & vm_ )
    :matroska_script_interpreter_common_c( log, vm_ )
    {
        ctx = ms_setup();
    }

    bool Interpret( const binary * p_command, size_t i_size ) override;
    void HandleMousePressed (unsigned x, unsigned y)
        {
            vm.HandleMouseClicked(x,y);
        }

    // MatroskaJS commands
    static const char* CMD_MS_GOTO_AND_PLAY;
    static const char* CMD_MS_LOG_MSG;
    static const char* CMD_MS_ADD_CHOICE;
    static const char* CMD_MS_COMMIT_CHOICES;
    static const char* CMD_MS_SET_CHOICE_TEXT;
    static const char* CMD_MS_SET_CHOICE_DEFAULT;
    static const char* CMD_MS_GET_CHOICE;
};

class matroska_js_codec_c : public matroska_script_codec_common_c
{
public:
    matroska_js_codec_c( struct vlc_logger *log, chapter_codec_vm & vm_, matroska_js_interpreter_c & interpreter_)
    :matroska_script_codec_common_c( log, vm_, MATROSKA_CHAPTER_CODEC_MATROSKA_JS)
    ,interpreter( interpreter_ )
    {}

    matroska_script_interpreter_common_c & get_interpreter() override
    {
        return interpreter;
    }

protected:
    matroska_js_interpreter_c & interpreter;
};

} // namespace

#endif // VLC_MKV_CHAPTER_COMMAND_SCRIPT_HPP_
