/*****************************************************************************
 * keystore.c: save and retrieve credentials from the keystore functions
 *****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 *
 * Authors: Maxime EVEN <maximeeven at proton.me>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifndef  _GNU_SOURCE
#   define  _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_meta.h>
#include <vlc_keystore.h>

#include "../vlc.h"
#include "../libs.h"


/****************************************************************************
 * Define : 
 ****************************************************************************/

#define VALUES_REINIT() values_reinit(ppsz_values)
#define VALUES_INSERT(i_key, psz_value) ppsz_values[i_key] = psz_value                 
#define GET_CREDENTIALS_FOR_PUSH() char *psz_server = get_cred( L ); \
    char *psz_protocol = get_cred( L ); \
    char *psz_password = get_cred( L ); \
    char *psz_username = get_cred( L );
#define GET_CREDENTIALS_FOR_FETCH() char *psz_server = get_cred( L ); \
    char *psz_protocol = get_cred( L );
#define PUSH_STRING_TO_LUA(s_key, string) lua_pushstring(L, string); \
    lua_setglobal(L, s_key);

static const char *ppsz_values[KEY_MAX];


/*****************************************************************************
 * Functions
 *****************************************************************************/

static void values_reinit( const char * ppsz_values[KEY_MAX] )
{
    memset(ppsz_values, 0, sizeof(const char *) * KEY_MAX);
}

static char *get_cred( lua_State *L ){
    char *return_string = lua_tostring(L, -1);
    lua_pop(L, 1);
    return return_string;
}

static int vlclua_push_credentials_to_keystore( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_keystore *p_keystore = vlc_keystore_create(p_this);
    vlc_keystore_entry *p_entries;

    GET_CREDENTIALS_FOR_PUSH();
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, psz_protocol);
    VALUES_INSERT(KEY_SERVER, psz_server);

    int i_entries = vlc_keystore_find(p_keystore, ppsz_values, &p_entries);
    if ( i_entries != 0 ) {
        vlc_keystore_remove(p_keystore,ppsz_values);
    }

    VALUES_INSERT(KEY_USER, psz_username);
    vlc_keystore_store(p_keystore, ppsz_values, (const uint8_t *)psz_password, -1, "libVLC");

    return 0;
}

static int vlclua_fetch_credentials_from_keystore( lua_State *L )
{
    vlc_object_t *p_this = vlclua_get_this( L );
    vlc_keystore *p_keystore = vlc_keystore_create(p_this);
    vlc_keystore_entry *p_entries;

    GET_CREDENTIALS_FOR_FETCH();
    VALUES_REINIT();
    VALUES_INSERT(KEY_PROTOCOL, psz_protocol);
    VALUES_INSERT(KEY_SERVER, psz_server);

    int i_entries = vlc_keystore_find(p_keystore, ppsz_values, &p_entries);
    if(i_entries == 1){
        PUSH_STRING_TO_LUA("s_keystore_username", (const char *)(&p_entries[0])->ppsz_values[1]);
        PUSH_STRING_TO_LUA("s_keystore_password", (const char *)(&p_entries[0])->p_secret);        
    }

    return 0;
}


/*****************************************************************************
 *
 *****************************************************************************/

static const luaL_Reg vlclua_keystore_reg[] = {
    { "push", vlclua_push_credentials_to_keystore },
    { "fetch", vlclua_fetch_credentials_from_keystore },

    { NULL, NULL }
};

void luaopen_keystore( lua_State *L )
{
    lua_newtable( L );
    luaL_register( L, NULL, vlclua_keystore_reg);
    lua_setfield( L, -2, "keystore" );
}
