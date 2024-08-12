// Copyright (C) 2024 VLC authors and VideoLAN
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// chapter_command_matroska_js.cpp : MatroskaJS implementation for Matroska Chapter Codecs
// Author: Khalid Masum <khalid.masum.92@gmail.com>


#include "chapter_command_matroska_js.hpp"
#include "virtual_segment.hpp"

#include "duktape/duktape.h"

duk_bool_t duk_check_timeout(void *udata) {
    if (udata == nullptr) return false;
    auto data = static_cast<mkv::matroska_js_interpreter_c*>(udata);
    return data->timed_out;
}

namespace mkv {

constexpr vlc_tick_t INTERPRETER_TIMEOUT = VLC_TICK_FROM_SEC(3);

//MatroskaJS
const char* matroska_js_interpreter_c::CMD_MS_GOTO_AND_PLAY = "GotoAndPlay";
const char* matroska_js_interpreter_c::CMD_MS_LOG_MSG = "LogMsg";
const char* matroska_js_interpreter_c::CMD_MS_ADD_CHOICE = "AddChoice";
const char* matroska_js_interpreter_c::CMD_MS_COMMIT_CHOICES = "CommitChoices";
const char* matroska_js_interpreter_c::CMD_MS_SET_CHOICE_TEXT = "SetChoiceText";
const char* matroska_js_interpreter_c::CMD_MS_SET_CHOICE_DEFAULT = "SetChoiceDefault";
const char* matroska_js_interpreter_c::CMD_MS_GET_CHOICE = "GetChoice";

static matroska_js_interpreter_c* receive_interpreter_object(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1,  DUK_HIDDEN_SYMBOL("interpreter_obj"));
    matroska_js_interpreter_c *interpretor = static_cast<matroska_js_interpreter_c*>( duk_get_pointer(ctx, -1));

    return interpretor;
}

bool matroska_js_interpreter_c:: gotoChapter(chapter_uid i_chapter_uid)
{
    virtual_segment_c *p_vsegment;
    virtual_chapter_c *p_vchapter = vm.FindVChapter( i_chapter_uid, p_vsegment );

    if ( p_vchapter == NULL )
        vlc_debug( l, "Chapter %" PRId64 " not found", i_chapter_uid);
    else
    {
        if ( !p_vchapter->EnterAndLeave( vm.GetCurrentVSegment()->CurrentChapter(), false ) )
        {
            vm.JumpTo( *p_vsegment, *p_vchapter );
        }
        return true;
    }

    return false;
}


/**
 * execute_GotoAndPlay:
 * Handles Matroska Script GotoAndPlay command by jumping to the
 * designated chapter.
 *
 * Takes an argument string containing,
 * GotoAndPlay Command Parameter:
 *
 * chapter_uid      string: The goto chapter uid.
 *
 */
bool matroska_js_interpreter_c::execute_GotoAndPlay(const std::string &arg)
{
    chapter_uid i_chapter_uid = atoll( arg.c_str() );
    bool b_result = gotoChapter(i_chapter_uid);

    return b_result;
}

duk_ret_t matroska_js_interpreter_c::js_execute_GotoAndPlay(duk_context *ctx)
{
    auto interpretor = receive_interpreter_object(ctx);

    if (!duk_is_string(ctx, 0))
    {
        vlc_debug(interpretor->l, "%s: First argument must be a string", CMD_MS_GOTO_AND_PLAY);
        return DUK_RET_TYPE_ERROR;
    }

    const char* arg = duk_to_string(ctx, 0);

    interpretor->execute_GotoAndPlay(arg);

    return 0;

}

/**
 * execute_LogMsg:
 * Handles Matroska Script LogMsg command by logging the passed
 * parameter.
 *
 * Takes an argument string containing,
 * LogMsg Command Parameter:
 *
 * arg      string: The string to be logged.
 *
 */
bool matroska_js_interpreter_c::execute_LogMsg(const std::string &arg)
{
    vlc_debug(l, "%s", arg.c_str());
    return true;
}

duk_ret_t matroska_js_interpreter_c::js_execute_LogMsg(duk_context *ctx)
{
    auto interpretor = receive_interpreter_object(ctx);

    if (!duk_is_string(ctx, 0))
    {
        vlc_debug(interpretor->l, "%s: First argument must be a string", CMD_MS_LOG_MSG);
        return DUK_RET_TYPE_ERROR;
    }

    const char* arg = duk_to_string(ctx, 0);
    interpretor->execute_LogMsg(arg);

    return 0;
}


/**
 * execute_AddChoice:
 * Handles Matroska Script AddChoice command. Which is to instruct
 * the MKV player to add choices to the choises list that will be
 * shown to the user later.
 *
 * Takes an argument string containing,
 * AddChoice command parameters:
 *
 * choice_uid           string: uid of the choice
 * chapte_group         string: choice group (default: null)
 */
bool matroska_js_interpreter_c::execute_AddChoice(const std::string &choice_uid, const std::optional <std::string> &choice_group)
{
    chapter_codec_vm::choice_text per_language_text;

    chapter_codec_vm::chapter_choice choice = {
        per_language_text, choice_group
    };

    choice_map[choice_uid] = choice;

    return true;
}

duk_ret_t matroska_js_interpreter_c::js_execute_AddChoice(duk_context *ctx)
{
    auto interpretor = receive_interpreter_object(ctx);

    if (!duk_is_string(ctx, 0))
    {
        vlc_debug(interpretor->l, "%s: First argument must be a string", CMD_MS_ADD_CHOICE);
        return DUK_RET_TYPE_ERROR;
    }

    chapter_codec_vm::choice_group group;

    if (duk_is_undefined(ctx, 1))
        group = std::nullopt;
    else if (!duk_is_string(ctx, 1))
        return DUK_RET_TYPE_ERROR;

    group = duk_to_string(ctx, 1);
    const char* choice_uid = duk_to_string(ctx, 0);
    interpretor->execute_AddChoice(choice_uid, group);

    return 0;
}

/**
 * execute_CommitChoices():
 * Handles Matroska Script CommitChoices command. Which is to instruct
 * the MKV player to display the previously added choices.
 */
void matroska_js_interpreter_c::execute_CommitChoices()
{
    if (choice_map.empty()){
        vlc_debug(l, "No choices to process");
        return;
    }

    const char* assume_language_from_vlc = "en"; // TODO: Get the chapter codec language from settings
    vm.AddChoices(choice_map);

// WIP:Just faking a display for now in console
    for (const auto & chapter_choice_pair : choice_map)
    {
        const auto & chapter_choice = chapter_choice_pair.second;
        const std::string choice_group = chapter_choice.group.value_or("Null");
        const std::string choice_uid = chapter_choice_pair.first;

        auto find_text = chapter_choice.per_language_text.find(assume_language_from_vlc);

        if (find_text == chapter_choice.per_language_text.end())
        {
            vlc_debug(l, "Unspecified choice text for uuid: %s, group: %s", choice_uid.c_str(), choice_group.c_str());
            continue;
        }

        const std::string text = find_text->second;

        vlc_debug(l, "Displaying choice with uuid: %s, string: %s, group: %s",
                 choice_uid.c_str(), text.c_str(), choice_group.c_str());
    }
}

duk_ret_t matroska_js_interpreter_c::js_execute_CommitChoices(duk_context *ctx)
{
    auto interpretor = receive_interpreter_object(ctx);
    interpretor->execute_CommitChoices();

    return 0;
}

/**
 * execute_SetChoiceText:
 *
 * uid      string: uid of the choice
 * text     string: text
 * lang     string: language
 */
bool matroska_js_interpreter_c::execute_SetChoiceText(const std::string &uid, const std::string &text, const std::string &lang)
{
    auto chapter_choice_pair = choice_map.find(uid);

    if (chapter_choice_pair == choice_map.end()) {
        vlc_debug(l, "The choice with uid '%s' does not exist", uid.c_str());
        return false;
    }

    chapter_choice_pair->second.per_language_text[lang] = text;

    return true;
}

duk_ret_t matroska_js_interpreter_c::js_execute_SetChoiceText(duk_context *ctx)
{
    auto interpretor = receive_interpreter_object(ctx);

    if (!duk_is_string(ctx, 0))
    {
        vlc_debug(interpretor->l, "%s: First argument must be a string", CMD_MS_SET_CHOICE_TEXT);
        return DUK_RET_TYPE_ERROR;
    }

    if (!duk_is_string(ctx, 1))
    {
        vlc_debug(interpretor->l, "%s: second argument must be a string", CMD_MS_SET_CHOICE_TEXT);
        return DUK_RET_TYPE_ERROR;
    }

    if (!duk_is_string(ctx, 2))
    {
        vlc_debug(interpretor->l, "%s: third argument must be a string", CMD_MS_SET_CHOICE_TEXT);
        return DUK_RET_TYPE_ERROR;
    }

    const char* uid = duk_to_string(ctx, 0);
    const char* text = duk_to_string(ctx, 1);
    const char* lang = duk_to_string(ctx, 2);

    interpretor->execute_SetChoiceText(uid, text, lang);

    return 0;

}

/**
 * execute_SetChoiceDefault:
 *
 * Set a default choice for the given group or uid.
 *
 */

void matroska_js_interpreter_c::execute_SetChoiceDefault(const choice_uid &uid, const choice_group &group)
{
    choice_map.SetSelected(uid, group);
}

duk_ret_t matroska_js_interpreter_c::js_execute_SetChoiceDefault(duk_context *ctx)
{
    if (!duk_is_string(ctx, 0))
        return DUK_RET_TYPE_ERROR;

    if (!duk_is_string(ctx, 1))
        return DUK_RET_TYPE_ERROR;

    const char* uid = duk_to_string(ctx, 0);
    const char* group = duk_to_string(ctx, 1);
    auto interpretor = receive_interpreter_object(ctx);
    interpretor->execute_SetChoiceDefault(uid, group);

    return  0;
}

/**
 * execute_GetChoice:
 *
 * Handles MatroskaJS GetChoice command.
 *
 * Returns:
 * chapter_uid  string: uid of the choice selected by the user
 *
 */

const std::optional<chapter_codec_vm::choice_uid> matroska_js_interpreter_c::execute_GetChoice(const choice_group &group)
{
    auto selected = choice_map.GetSelected(group);
    auto choice = vm.GetChoice(group);
    return choice;
}

duk_ret_t matroska_js_interpreter_c::js_execute_GetChoice(duk_context *ctx)
{
    choice_group group;
    auto interpretor = receive_interpreter_object(ctx);

    if (duk_is_null_or_undefined(ctx, 0))
        group = std::nullopt;
    else if (duk_is_string(ctx, 0))
        group = duk_to_string(ctx, 0);
    else
    {
        vlc_debug(interpretor->l, "%s: First argument must be a string", CMD_MS_GET_CHOICE);
        return DUK_RET_TYPE_ERROR;
    }

    const std::optional<choice_uid> selected_uid = interpretor->execute_GetChoice(group);

    if (selected_uid == std::nullopt)
        duk_push_undefined(ctx);
    else duk_push_string(ctx, selected_uid->c_str());

    //Return Stack Top (ie. undefined or choice-uid string in this case)
    return 1;
}


void matroska_js_interpreter_c::on_timeout()
{
    vlc_error(l,"Script taking too long (%" PRId64 ") to execute, stopping", SEC_FROM_VLC_TICK(INTERPRETER_TIMEOUT));
    timed_out = true;
}

void matroska_js_interpreter_c::fatal_error_handler(void *udata, const char* msg)
{
    auto metadata_obj = static_cast<matroska_js_interpreter_c*> (udata);
    vlc_error( metadata_obj->l, "DUKTAPE: FATAL ERROR OCCURED DURING JS EXECUTION:\n %s", msg );
    throw std::runtime_error("DUKTAPE FATAL");
}


duk_context* matroska_js_interpreter_c::ms_setup()
{
    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, this, fatal_error_handler);

    duk_push_c_function(ctx, js_execute_GotoAndPlay, 1);
    duk_put_global_string(ctx, CMD_MS_GOTO_AND_PLAY);

    duk_push_c_function(ctx, js_execute_LogMsg, 1);
    duk_put_global_string(ctx, CMD_MS_LOG_MSG);

    duk_push_c_function(ctx, js_execute_AddChoice, 2);
    duk_put_global_string(ctx, CMD_MS_ADD_CHOICE);

    duk_push_c_function(ctx, js_execute_CommitChoices, 0);
    duk_put_global_string(ctx, CMD_MS_COMMIT_CHOICES);

    duk_push_c_function(ctx, js_execute_SetChoiceDefault, 2);
    duk_put_global_string(ctx, CMD_MS_SET_CHOICE_DEFAULT);

    duk_push_c_function(ctx, js_execute_SetChoiceText, 3);
    duk_put_global_string(ctx, CMD_MS_SET_CHOICE_TEXT);

    duk_push_c_function(ctx, js_execute_GetChoice, 1);
    duk_put_global_string(ctx, CMD_MS_GET_CHOICE);

    duk_push_global_object(ctx);
    duk_push_pointer(ctx, this);
    duk_put_prop_literal(ctx, -2, DUK_HIDDEN_SYMBOL("interpreter_obj"));

    return ctx;
}

// see https://datatracker.ietf.org/doc/draft-ietf-cellar-chapter-codecs
//  for a description of existing commands
bool matroska_js_interpreter_c::Interpret( const binary * p_command, size_t i_size )
{
    bool success = false;
    std::string sz_command( reinterpret_cast<const char*> (p_command), i_size );
    vlc_timer_t timer;

    vlc_debug( l, "command input : %s", sz_command.c_str() );
    try {
        if (vlc_timer_create(&timer, [](void *p) {
            auto p_this = static_cast<matroska_js_interpreter_c *>(p);
            p_this->on_timeout();
            }, this))
        {
            vlc_debug(l, "Timer initialization failed");
            return false;
        }
        timed_out = false;
        vlc_timer_schedule(timer, false, INTERPRETER_TIMEOUT, VLC_TIMER_FIRE_ONCE);
        success = !duk_peval_lstring(ctx, sz_command.c_str(), sz_command.size());
        vlc_timer_destroy(timer);

        if (timed_out)
        {
            vlc_error(l, "Execution TimedOut!\n%s", duk_safe_to_stacktrace(ctx, -1));
            return false;
        }

        if (!success)
        {
            vlc_error(l, "duktape:: Evaluation Failed!\n%s", duk_safe_to_stacktrace(ctx, -1));
            return false;
        }
    } catch (std::exception e){
        vlc_error(l, "duktape:: Evaluation Failed!\n%s", duk_safe_to_stacktrace(ctx, -1));
        return false;
    }
    vlc_debug(l, "duktape:: Evaluation complete");

    return true;
}

} // namespace
